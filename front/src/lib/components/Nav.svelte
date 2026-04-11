<script lang="ts">
	let { currentPath, pages } = $props();
</script>

<nav class="nav">
	<ul class="nav__list">
		{#each pages as page}
			<li class="nav__item">
				{#if page.path === currentPath}
					<span class="nav__link nav__link--current">{page.name}</span>
				{:else}
					<a href={page.path} class="nav__link">{page.name}</a>
				{/if}
			</li>
		{/each}
	</ul>
</nav>

<style lang="scss">
	@use './../styles/variables' as *;
	@use './../styles/mixins.scss' as *;

	.nav {
		position: fixed;
		padding: 8px;
		left: 50%;
		bottom: calc($padding-body-y / 4);
		translate: -50% 0;
		width: calc(100% - 2 * $padding-body-x);
		max-width: $max-width-body - $padding-body-x * 2;
		height: 75px;
		z-index: 100;

		@include glass;

		&__list {
			display: flex;
			gap: 12px;
			align-items: center;
			height: 100%;
		}

		&__item {
			flex: 1;
			height: 100%;
		}

		&__link {
			height: 100%;
			width: 100%;
			display: flex;
			justify-content: center;
			align-items: center;
			text-align: center;
			border-radius: $border-radius - 4px;
			border: 0;

			&--current,
			&:hover,
			&:active {
				background-color: rgba($white, 0.9);
			}
		}
	}
</style>
