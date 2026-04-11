<script lang="ts">
	import ArticlePreview from '$lib/components/ArticlePreview.svelte';
	import ArticlesList from '$lib/components/ArticlesList.svelte';
	import Button from '$lib/components/Button.svelte';
	import Card from '$lib/components/Card.svelte';
	import VODsButton from '$lib/components/VODsButton.svelte';
	import type { Article } from '../../types';

	const latestArticle: Article | null = {
		title: 'Moltbook, or when the AI bubble gets too big',
		excerpt:
			'Because we never tire of new AI-boosted features, Figma announced yesterday the vectorization of raster images. Now, I don’t know if you’ve ever tried to do this, but every time, it wasn’t great. This feature is available for paid plans and uses AI credits.',
		editionNumber: 72,
		publishedDate: new Date('02/08/2026'),
		content: [],
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

	const articles: Article[] = [
		{
			title: 'Moltbook, or when the AI bubble gets too big',
			excerpt:
				'Because we never tire of new AI-boosted features, Figma announced yesterday the vectorization of raster images. Now, I don’t know if you’ve ever tried to do this, but every time, it wasn’t great. This feature is available for paid plans and uses AI credits.',
			editionNumber: 72,
			publishedDate: new Date('02/08/2026'),
			content: [],
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
		},
		{
			title: 'ChatGPT wants to guess your age',
			subtitle: 'The coming back of Date.now()!',
			excerpt:
				'Welcome back to your favorite newsletter! Thank you for your patience; I needed this break.',
			editionNumber: 69,
			publishedDate: new Date('01/25/2026'),
			content: [],
			tags: [
				{
					name: 'Cybersecurity',
					color: {
						r: 140,
						g: 30,
						b: 20
					}
				},
				{
					name: 'Web',
					color: {
						r: 50,
						g: 230,
						b: 100
					}
				}
			]
		},
		{
			title: 'Programming languages obsolete end of 2026!',
			excerpt:
				'Welcome back to your favorite newsletter! Thank you for your patience; I needed this break.',
			editionNumber: 69,
			publishedDate: new Date('01/25/2026'),
			content: [],
			tags: [
				{
					name: 'Cybersecurity',
					color: {
						r: 140,
						g: 30,
						b: 20
					}
				},
				{
					name: 'Web',
					color: {
						r: 50,
						g: 230,
						b: 100
					}
				}
			]
		}
	];
</script>

<Card tag="section" customClass="archive">
	<h1>Passed Issues</h1>

	<p>You can find all the past issues of <strong>Date.now()</strong></p>

	<ul class="archive__links">
		<li><Button variant="primary">Subscribe</Button></li>
		<li><VODsButton /></li>
	</ul>
</Card>

<Card tag="section" customClass="archive__latest">
	<h2>Latest Issue</h2>

	{#if latestArticle !== null}
		<ArticlePreview size="large" article={latestArticle} />
	{:else}
		<p class="archive__latest__empty">No issue yet, sorry 👀😣</p>
	{/if}
</Card>

<ArticlesList {articles} title="All past issues" />

<style lang="scss">
	.archive {
		&__links {
			display: flex;
			gap: 8px;
		}

		&__latest {
			&__empty {
				opacity: 0.75;
				margin: 0;
			}
		}
	}
</style>
