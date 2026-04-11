<script lang="ts">
	import Card from '$lib/components/Card.svelte';
	import Tag from '$lib/components/Tag.svelte';
	import { format } from 'date-fns';
	import type { Article as ArticleType } from '../../types';
	import Article from '$lib/components/Article.svelte';
	import BlockRenderer from '$lib/components/blockRenderer/BlockRenderer.svelte';

	const article: ArticleType = {
		title: 'Moltbook, or when the AI bubble gets too big',
		excerpt:
			'Because we never tire of new AI-boosted features, Figma announced yesterday the vectorization of raster images. Now, I don’t know if you’ve ever tried to do this, but every time, it wasn’t great. This feature is available for paid plans and uses AI credits.',
		editionNumber: 72,
		publishedDate: new Date('02/08/2026'),
		content: [
			{
				tag: 'p',
				content:
					'It’s the party for “pre-Terminator” scenarios (I just invented this term, no idea if it’s correct) this year! And we’re only in the second month of 2026. Happy New Year still! After the ClawdBot scandal, I present to you our next buzz-wannabe, Moltbook. Get the popcorn ready, you’ll need it.'
			},
			{
				tag: 'img',
				attributes: {
					src: 'https://substackcdn.com/image/fetch/$s_!Wjan!,w_1456,c_limit,f_webp,q_auto:good,fl_progressive:steep/https%3A%2F%2Fsubstack-post-media.s3.amazonaws.com%2Fpublic%2Fimages%2F9f4855d5-f6aa-4d43-b9cb-4e7b21186352_1137x759.png',
					alt: 'Moltbook logo'
				}
			},
			{
				tag: 'p',
				children: [
					{
						tag: 'span',
						content: `Sold and touted as a social network completely managed by AI agents, Moltbook is a sort of Reddit where posting and commenting is done only by API call. So, no GUI for posting messages and comments. Logical, because AI agents don’t need one, you understand? However, a journalist from Wired media, a non-technical person I should note, had fun infiltrating the network and seeing if a human is capable of posting on the Reddit-like. Attention, surprise, it’s completely possible (obviously), but what’s interesting is that according to their testimony, it’s obvious that humans are behind the responses. Maybe it really is an agent posting and commenting, but the content itself would be heavily influenced by a human entity. `
					},
					{
						tag: 'a',
						content: 'Here',
						attributes: {
							href: 'https://www.moltbook.com/post/c57ac905-725d-46d8-8374-87c43ea95890'
						}
					},
					{
						tag: 'span',
						content: ' is one of my favorite post.'
					}
				]
			},
			{
				tag: 'p',
				content:
					'Moreover, a security reminder, Moltbook is a data sieve. More than 1.5 million API tokens, 35,000 email addresses and messages with AI agents are exposed. Manna from heaven for malicious people. All this because of a misconfigured Supabase backend...'
			},
			{
				tag: 'p',
				content:
					'Another creepy AI service we discovered during the stream; RentAHuman. Please pay attention that you will be paid with cryptomonney, which is really not good. Pay attention to your data.'
			}
		],
		tags: [
			{
				name: 'AI',
				color: {
					r: 100,
					g: 75,
					b: 230
				}
			}
		]
	};
</script>

<div class="issue">
	<Card customClass="issue__header">
		<h1 class="issue__title">{article.title}</h1>

		<p class="issue__metadata">
			<Tag tag="span">Issue #{article.editionNumber}</Tag>
			<span>{format(article.publishedDate, 'dd/MM/yyyy')}</span>
		</p>

		<ul class="issue__tags">
			{#each article.tags as tag}
				<Tag tag="li" --color={`${tag.color.r}, ${tag.color.g}, ${tag.color.b}`}>{tag.name}</Tag>
			{/each}
		</ul>
	</Card>

	<img
		class="issue__cover"
		alt="Issue's cover"
		src={article.coverURL || '/article-cover-placeholder.png'}
	/>

	<Article customClass="issue__content">
		<BlockRenderer blocks={article.content} />
	</Article>
</div>

<style lang="scss">
	@use '../../lib/styles/variables' as *;
	@use '../../lib/styles/mixins' as *;

	:global(.issue__header) {
		order: 2;
		display: flex;
		flex-wrap: wrap;
		justify-content: space-between;
		align-items: center;
		gap: 12px;
	}

	:global(.issue__content) {
		order: 3;
	}

	.issue {
		display: flex;
		flex-direction: column;
		gap: 12px;

		&__title {
			width: 100%;
			margin: 0;
			order: 1;
		}

		&__metadata {
			display: flex;
			align-items: center;
			gap: 8px;
			margin: 0;
		}

		&__cover {
			order: 1;
			height: 200px;
			object-fit: cover;
			object-position: center;
			border-radius: $border-radius;
			box-shadow: $box-shadow-bold;
			border: 1px solid rgba($white, 0.5);

			@include md {
				height: 300px;
			}

			@include lg {
				height: 400px;
			}
		}
	}
</style>
