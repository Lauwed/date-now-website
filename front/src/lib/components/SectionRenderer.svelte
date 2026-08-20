<script lang="ts">
	import type { RenderedSection } from '../../routes/issue/[slug]/+page.server';

	interface Props {
		sections: RenderedSection[];
	}

	let { sections }: Props = $props();
</script>

<div class="sections">
	{#each sections as section (section.id)}
		<section class="sections__item">
			{#if section.type === 'TEXT'}
				{#if section.textBodyHtml}
					<!-- eslint-disable-next-line svelte/no-at-html-tags -->
					{@html section.textBodyHtml}
				{/if}
			{:else}
				<h2 class="sections__title">{section.categoryName}</h2>

				<ul class="sections__articles">
					{#each section.articles as article (article.id)}
						<li class="sections__article">
							<h3 class="sections__article-title">{article.title}</h3>

							<a class="sections__source" href={article.sourceUrl} rel="noreferrer noopener nofollow" target="_blank">
								{article.sourceName}
							</a>

							<!-- eslint-disable-next-line svelte/no-at-html-tags -->
							{@html article.summaryHtml}
						</li>
					{/each}
				</ul>
			{/if}
		</section>
	{/each}
</div>

<style lang="scss">
	@use './../styles/variables' as *;

	.sections {
		display: flex;
		flex-direction: column;
		gap: 32px;

		&__title {
			margin: 0 0 12px;
		}

		&__articles {
			list-style: none;
			margin: 0;
			padding: 0;
			display: flex;
			flex-direction: column;
			gap: 20px;
		}

		&__article-title {
			margin: 0 0 4px;
		}

		&__source {
			font-size: 0.875rem;
			opacity: 0.8;
		}
	}
</style>
